using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class bankDataManager
    {
        //bankViewModel
        //public long id { get; set; }
        //public long? min_limit { get; set; }
        //public long? max_limit { get; set; }
        //public DateTime? add_ts { get; set; }
        //public string name { get; set; }
        //public int? rate { get; set; }
        //public long? merchant_id { get; set; }
        //public long? terminal_id { get; set; }
        //public int? port { get; set; }
        //public long? month_limit { get; set; }
        //public string offer_link { get; set; }
        //public int status { get; set; }
        //public string bin_code { get; set; }
        //public long? icon_id { get; set; }

        public static void Add(bankViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Modify(bankViewModel model, RAD_PAYEntities db)
        {

        }

        public static void Delete(bankViewModel model, RAD_PAYEntities db)
        {

        }

        public static List<bankViewModel> Get(bankViewModel model, RAD_PAYEntities db)
        {
            return new List<bankViewModel>();
        }
    }
}