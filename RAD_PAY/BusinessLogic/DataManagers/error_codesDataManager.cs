using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class error_codesDataManager
    {
        //error_codesViewModel
        //public long       id          { get; set; }
        //public int?       value       { get; set; }
        //public string     message_eng { get; set; }
        //public string     message_rus { get; set; }
        //public string     message_uzb { get; set; }
        //public int?       ex_id       { get; set; }

        public static void Add(error_codesViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new error_codes
            {
                id              = model.id          ,
                value           = model.value       ,
                message_eng     = model.message_eng ,
                message_rus     = model.message_rus ,
                message_uzb     = model.message_uzb ,
                ex_id           = model.ex_id       ,
            };

            db.error_codes.Add(dbmodel);
        }

        public static void Modify(error_codesViewModel model, RAD_PAYEntities db)
        {
            var result = db.error_codes.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id                   ;
                    dbmodel.value = model.value             ;
                    dbmodel.message_eng = model.message_eng ;
                    dbmodel.message_rus = model.message_rus ;
                    dbmodel.message_uzb = model.message_uzb ;
                    dbmodel.ex_id = model.ex_id;
                }
            }
        }

        public static void Delete(error_codesViewModel model, RAD_PAYEntities db)
        {
            var result = db.error_codes.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.error_codes.Remove(dbmodel);
                }
            }
        }

        public static List<error_codesViewModel> Get(error_codesViewModel model, RAD_PAYEntities db)
        {
            List<error_codesViewModel> list = null;

            var query = from resmodel in db.error_codes
                        select new error_codesViewModel
                        {
                            id = resmodel.id,
                            value = resmodel.value,
                            message_eng = resmodel.message_eng,
                            message_rus = resmodel.message_rus,
                            message_uzb = resmodel.message_uzb,
                            ex_id = resmodel.ex_id,
                        };

            list = query.ToList();

            return list;
        }
    }
}