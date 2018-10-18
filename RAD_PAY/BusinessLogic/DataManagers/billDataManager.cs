using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class billDataManager
    {
        //billViewModel
        //public long       bill_id { get; set; }
        //public decimal    deposit { get; set; }
        //public long       uid     { get; set; }

        public static void Add(billViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new bill
            {
                bill_id = model.bill_id,
                deposit = model.deposit,
                uid     = model.uid    ,
            };

            db.bills.Add(dbmodel);
        }

        public static void Modify(billViewModel model, RAD_PAYEntities db)
        {
            var result = db.bills.Where(z => z.bill_id == model.bill_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.bill_id = model.bill_id ;
                    dbmodel.deposit = model.deposit ;
                    dbmodel.uid = model.uid;
                }
            }
        }

        public static void Delete(billViewModel model, RAD_PAYEntities db)
        {
            var result = db.bills.Where(z => z.bill_id == model.bill_id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.bills.Remove(dbmodel);
                }
            }
        }

        public static List<billViewModel> Get(billViewModel model, RAD_PAYEntities db)
        {
            List<billViewModel> list = null;

            var query = from resmodel in db.bills
                        select new billViewModel
                        {
                            bill_id = resmodel.bill_id,
                            deposit = resmodel.deposit,
                            uid     = resmodel.uid,
                        };

            list = query.ToList();

            return list;
        }
    }
}