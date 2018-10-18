using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class activate_codeDataManager
    {
        //activate_codeViewModel
        //public long       id          { get; set; }
        //public string     phone       { get; set; }
        //public string     code        { get; set; }
        //public DateTime?  add_ts      { get; set; }
        //public string     dev_id      { get; set; }
        //public bool       valid       { get; set; }
        //public int        kind        { get; set; }
        //public long?      other_id    { get; set; }
        //public int?       lives       { get; set; }

        public static void Add(activate_codeViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new activate_code
            {
                id = model.id,
                phone = model.phone,
                code = model.code,
                add_ts = model.add_ts,
                dev_id = model.dev_id,
                valid = model.valid,
                kind = model.kind,
                other_id = model.other_id,
                lives = model.lives
            };

            db.activate_code.Add(dbmodel);
        }

        public static void Modify(activate_codeViewModel model, RAD_PAYEntities db)
        {
            var result = db.activate_code.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.phone = model.phone;
                    dbmodel.code = model.code;
                    dbmodel.add_ts = model.add_ts;
                    dbmodel.dev_id = model.dev_id;
                    dbmodel.valid = model.valid;
                    dbmodel.kind = model.kind;
                    dbmodel.other_id = model.other_id;
                    dbmodel.lives = model.lives;
                }
            }

        }

        public static void Delete(activate_codeViewModel model, RAD_PAYEntities db)
        {
            var result = db.activate_code.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.activate_code.Remove(dbmodel);
                }
            }
        }

        public static List<activate_codeViewModel> Get(activate_codeViewModel model, RAD_PAYEntities db)
        {
            List<activate_codeViewModel> list = null;

            var query = from resmodel in db.activate_code
                        select new activate_codeViewModel {
                            id       = resmodel.id       ,
                            phone    = resmodel.phone    ,
                            code     = resmodel.code     ,
                            add_ts   = resmodel.add_ts   ,
                            dev_id   = resmodel.dev_id   ,
                            valid    = resmodel.valid    ,
                            kind     = resmodel.kind     ,
                            other_id = resmodel.other_id ,
                            lives    = resmodel.lives    ,
                        };

            list = query.ToList();

            return list;
        }
    }
}